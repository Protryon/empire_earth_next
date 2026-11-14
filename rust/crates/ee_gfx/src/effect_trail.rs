//! The 2D winter breath (kind 0x16, `EEE2DWinterBreath`) and 2D projectile (0x30, `EEE2DLinearProjectile`) whole in
//! Rust on `EEE2DRust` (R4.4, twentieth round), their classes deleted. The C++ constructors read the shooter and the
//! target (the weapon tag point, the target's model point, the breath's mouth point and the unit's scaled location)
//! and hand them over in the record; the rest is here.

use ee_ge::types::{Ge3DLine, Ge3DPoint, GeVector};
use ee_ge::um;

use crate::effect_2d::Effect2DHost;
use crate::effect_rising::{EeGfxEffect3DOp, EeGfxGraphicFacts};
use crate::effect_sprite::EeGfxEffectSprite;
use crate::effect_state::{identity, load_point, random, save_point, EeGfxEffectRecord, EffectFile, OK};
use crate::effects::EeGfxColor;
use crate::lighting::EeGfxLighting;
use crate::raster::{Raster, BLEND_ONE, EE_GFX_SPRITE_BLENDED, ZBUFFER_TEST_ONLY};

/// `kEffect2DWinterBreath`, `kEffect2DLinearProjectile`.
pub const EE_GFX_EFFECT_KIND_WINTER_BREATH: i32 = 0x16;
pub const EE_GFX_EFFECT_KIND_LINEAR_PROJECTILE: i32 = 0x30;

/// The breath's rise per ms (before the 0.04 scale).
const BREATH_RISE: f32 = 0.0007;

/// The record's sprite state.
///
/// # Safety
/// `r` is a live record; the reference does not live across a host call.
unsafe fn sp<'a>(r: *mut EeGfxEffectRecord) -> &'a mut EeGfxEffectSprite {
    // SAFETY: the function's contract
    unsafe { &mut (*r).sprite }
}

/// `SetPosition(p)`.
///
/// # Safety
/// As `sp`.
unsafe fn set_position<H: Effect2DHost>(r: *mut EeGfxEffectRecord, p: Ge3DPoint, h: &mut H) {
    // SAFETY: the function's contract
    unsafe { (*r).position = p };
    h.move_to_point();
}

fn alpha_ref(g: &EeGfxGraphicFacts) -> f32 {
    if g.has_object {
        g.alpha_ref
    } else {
        0.0
    }
}

/// The record's line (the projectile's path: `start` and the direction in `velocity`).
fn line(s: &EeGfxEffectSprite) -> Ge3DLine {
    Ge3DLine { start: s.start, direction: GeVector { x: s.velocity.x, y: s.velocity.y, z: s.velocity.z } }
}

/// The winter breath's and projectile's virtuals.
///
/// # Safety
/// As `effect_2d::effect2d`.
pub unsafe fn trail<H: Effect2DHost, R: Raster>(
    op: EeGfxEffect3DOp,
    r: *mut EeGfxEffectRecord,
    g: &EeGfxGraphicFacts,
    h: &mut H,
    x: &mut R,
) -> i32 {
    // SAFETY (every access below): the function's contract; no reference to the record lives across a host call
    let breath = unsafe { (*r).kind } == EE_GFX_EFFECT_KIND_WINTER_BREATH;
    match op {
        // 0xe819b: the C++ put the mouth (or tag) point in `start`, the unit's scaled location in the aged
        // transformation, whether a tag list was found in `flag` and whether the unit moves in `shrinking`: the
        // timings (none without the tag list; shorter and faster on the move), the facing (0, 1, -1) through the
        // location, the place, the radius, a random rotation
        EeGfxEffect3DOp::CreateBetween if breath => {
            let (p, t, tagged, moving) = unsafe {
                let s = sp(r);
                let stash = (s.start, (*r).aged.transformation, s.flag, s.shrinking);
                s.start = Ge3DPoint::default();
                s.flag = false;
                s.shrinking = false;
                (*r).aged.transformation = identity();
                stash
            };
            let s = unsafe { sp(r) };
            s.duration = g.duration;
            s.speed = g.param1;
            s.slow_down = g.param2;
            s.fade = g.param3;
            if !tagged {
                s.fade = 0.0;
                s.speed = 0.0;
                s.slow_down = 0.0;
                s.duration = 0;
            }
            if moving {
                s.duration = um::lrintf(s.duration as f32 * 0.55) as u32;
                s.speed *= 1.05;
                s.slow_down *= 1.15;
            }
            let mut v = t.apply_vector(GeVector { x: 0.0, y: 1.0, z: -1.0 });
            v.normalize();
            s.velocity = Ge3DPoint { x: v.x, y: v.y, z: v.z };
            unsafe {
                set_position(r, p, h);
                (*r).bounding_radius = g.size;
            }
            let rotation = random() * 6.283_185_5;
            let s = unsafe { sp(r) };
            s.age = 0.0;
            s.rotation = rotation;
            0
        }
        // 0xe2f58 / 0xe31dd: the C++ put the weapon point in `start` and the target in `draw_position`: the line from
        // one to the other, at its start
        EeGfxEffect3DOp::CreateBetween => {
            let (start, end) = unsafe { (sp(r).start, sp(r).draw_position) };
            unsafe {
                let s = sp(r);
                s.age = 0.0;
                s.draw_age = 0.0;
                s.draw_position = Ge3DPoint::default();
                s.velocity = Ge3DPoint { x: end.x - start.x, y: end.y - start.y, z: end.z - start.z };
                set_position(r, start, h);
            }
            0
        }
        // 0xe6b62 (the muzzle blast's) / 0xe34c5: the media; the projectile at its first frame, its speed the duration
        // over the path's length
        EeGfxEffect3DOp::SetUp => {
            let res = h.object_media();
            if !breath {
                let first = h.frames(0).start;
                let s = unsafe { sp(r) };
                s.frame = first;
                s.time = 0.0;
                s.draw_frame = 0;
                unsafe { (*r).bounding_radius = g.size };
                let s = unsafe { sp(r) };
                let duration = g.duration as f32 * 0.00001;
                s.speed = duration / line(s).direction.magnitude();
            }
            res
        }
        // 0xe844d / 0xe3504: the age (the path's progress and frame) and position for the draw, render queue 4
        EeGfxEffect3DOp::Queue => {
            let res = h.base_queue();
            if res == 0 {
                unsafe {
                    let s = sp(r);
                    s.draw_age = s.age;
                    if !breath {
                        s.draw_frame = s.frame;
                    }
                    s.draw_position = (*r).position;
                }
                h.queue(4);
            }
            res
        }
        EeGfxEffect3DOp::DrawPass6 if breath => unsafe { breath_draw(r, g, x) },
        // 0xe36a0: the additive fog-lit sprite at the drawn position
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
            let mut t = camera;
            t.m[0][3] = p.x;
            t.m[1][3] = p.y;
            t.m[2][3] = p.z;
            x.model_transform(model, &t);
            x.model_scale(model, g.size);
            x.model_prepare(model);
            let res = x.model_draw(model, 1.0);
            x.set_fog(fog);
            res
        }
        // 0xe88b5: moves along the slowing direction with the wind, rising; 7 once over
        EeGfxEffect3DOp::Step if breath => {
            let wind = h.wind();
            let elapsed = g.tick_time;
            let s = unsafe { sp(r) };
            let mut step = elapsed * s.speed / 1000.0;
            s.age += elapsed;
            if !(s.age <= s.duration as f32) {
                return 7;
            }
            let mut fade = 1.0 - s.age / s.slow_down;
            if fade < 0.0 {
                fade = 0.0;
            }
            step = fade * fade * step;
            let (v, at) = (s.velocity, unsafe { (*r).position });
            let p = Ge3DPoint {
                x: v.x * step + wind.x * elapsed * 0.04 + at.x,
                y: v.y * step + wind.y * elapsed * 0.04 + at.y,
                z: (wind.z + BREATH_RISE) * elapsed * 0.04 + v.z * step + at.z,
            };
            unsafe { set_position(r, p, h) };
            0
        }
        // 0xe3840: along the path, looping the animation; 7 at its end
        EeGfxEffect3DOp::Step => {
            let (frame, time) = unsafe { (sp(r).frame, sp(r).time) };
            let frames = h.frames(0);
            let t = unsafe {
                sp(r).age += g.tick_time * sp(r).speed;
                sp(r).age
            };
            if !(t <= 1.0) {
                return 7;
            }
            let mut time = time + g.tick_time;
            let mut frame = frame;
            if time >= frames.time as f32 {
                let n = um::lrintf(time / frames.time as f32) as u32;
                time -= n.wrapping_mul(frames.time) as f32;
                frame = n.wrapping_add(frame) % frames.count;
            }
            let s = unsafe { sp(r) };
            s.time = time;
            s.frame = frame;
            let p = line(s).point_at(t);
            unsafe { set_position(r, p, h) };
            0
        }
        _ => 0,
    }
}

/// 0xe84c7: up to three turned sprites back along the direction (the later after their delays), growing with age and
/// fading by the breath's fade; 0.
///
/// # Safety
/// As `sp`.
unsafe fn breath_draw<R: Raster>(r: *mut EeGfxEffectRecord, g: &EeGfxGraphicFacts, x: &mut R) -> i32 {
    // SAFETY: the function's contract
    let s = unsafe { *sp(r) };
    let white = EeGfxColor { red: 1.0, green: 1.0, blue: 1.0 };
    let mut p = s.draw_position;
    let age = s.draw_age / s.duration as f32;
    let size = (g.size - g.start_size) * age + g.start_size;
    let size2 = size * 0.95;
    let size3 = size * 0.85;
    let alpha = 1.0 - (age * s.fade + (1.0 - s.fade));
    let delay = s.draw_age - g.time1 as f32;
    let delay2 = s.draw_age - g.time1.wrapping_mul(2) as f32;
    let offset = -g.alpha;
    let reference = alpha_ref(g);
    x.sprite(EE_GFX_SPRITE_BLENDED, &p, white, s.rotation, size, alpha, 1.0, reference);
    if !(delay >= 0.0) {
        return 0;
    }
    p.x += s.velocity.x * offset;
    p.y += s.velocity.y * offset;
    p.z += s.velocity.z * offset;
    x.sprite(EE_GFX_SPRITE_BLENDED, &p, white, s.rotation + 1.047_197_6, size2, alpha, 1.0, reference);
    if !(delay2 >= 0.0) {
        return 0;
    }
    p.x += s.velocity.x * offset;
    p.y += s.velocity.y * offset;
    p.z += s.velocity.z * offset;
    x.sprite(EE_GFX_SPRITE_BLENDED, &p, white, s.rotation + 2.094_395_2, size3, alpha, 1.0, reference);
    0
}

/// Reads or writes one value (the files' chains stop at the first error).
macro_rules! chain {
    ($res:ident, $e:expr) => {
        if $res == OK {
            $res = $e;
        }
    };
}

/// `Load` (`load`) or `Save` after `EEEffect2D`'s (whose result each ignores): the breath's direction, age, rotation,
/// drawn place and age, timings and rotation again (0xe85f1 / 0xe8725); the projectile's progress, speed, path, time,
/// frame and path again (0xe3561 / 0xe3780).
pub fn trail_file<H: Effect2DHost, F: EffectFile>(load: bool, r: &mut EeGfxEffectRecord, f: &mut F, h: &mut H) -> i32 {
    let breath = r.kind == EE_GFX_EFFECT_KIND_WINTER_BREATH;
    let s = &mut r.sprite;
    let mut res = OK;
    if load {
        h.object_media();
        if breath {
            chain!(res, load_point(f, &mut s.velocity));
            chain!(res, f.read_f32(&mut s.age));
            chain!(res, f.read_f32(&mut s.rotation));
            chain!(res, load_point(f, &mut s.draw_position));
            chain!(res, f.read_f32(&mut s.draw_age));
            chain!(res, f.read_u32(&mut s.duration));
            chain!(res, f.read_f32(&mut s.fade));
            chain!(res, f.read_f32(&mut s.speed));
            chain!(res, f.read_f32(&mut s.slow_down));
            chain!(res, f.read_f32(&mut s.rotation));
        } else {
            chain!(res, f.read_f32(&mut s.age));
            chain!(res, f.read_f32(&mut s.speed));
            chain!(res, load_point(f, &mut s.start));
            chain!(res, load_point(f, &mut s.velocity));
            chain!(res, f.read_f32(&mut s.time));
            chain!(res, f.read_u32(&mut s.frame));
            chain!(res, load_point(f, &mut s.start));
            chain!(res, load_point(f, &mut s.velocity));
        }
    } else if breath {
        chain!(res, save_point(f, &s.velocity));
        chain!(res, f.write_f32(s.age));
        chain!(res, f.write_f32(s.rotation));
        chain!(res, save_point(f, &s.draw_position));
        chain!(res, f.write_f32(s.draw_age));
        chain!(res, f.write_u32(s.duration));
        chain!(res, f.write_f32(s.fade));
        chain!(res, f.write_f32(s.speed));
        chain!(res, f.write_f32(s.slow_down));
        chain!(res, f.write_f32(s.rotation));
    } else {
        chain!(res, f.write_f32(s.age));
        chain!(res, f.write_f32(s.speed));
        chain!(res, save_point(f, &s.start));
        chain!(res, save_point(f, &s.velocity));
        chain!(res, f.write_f32(s.time));
        chain!(res, f.write_u32(s.frame));
        chain!(res, save_point(f, &s.start));
        chain!(res, save_point(f, &s.velocity));
    }
    res
}

#[cfg(test)]
#[path = "effect_trail_tests.rs"]
mod tests;
