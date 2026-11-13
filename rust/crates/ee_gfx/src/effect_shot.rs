//! The shots whole in Rust: the pulse cannon ball (kind 0x43), the tracers (0x21) and the shells (0x26)
//! (`EEE3DPulseCannonBall`, `EEE3DTracers`, `EEE3DShells`, deleted): a model flying along a line from the shooter's
//! weapon point (the C++ constructor's: `EECombatEffectWeaponPoint`) to its end, facing along it, at the effect's
//! duration value over the path's length per millisecond. The ball pulses its scale every tenth of the way and
//! grows as an explosion with its glow model past the end (7 at 1.5); a tracer places its impact effect and reports
//! the hit at the end (7); a shell draws alpha blended in pass 5 as well. All on `EEE3DRust` like
//! `effect_rising.rs`, the record reached through its raw pointer field by field as there.

use ee_ge::types::{Ge3DLine, Ge3DPoint, GeVector};
use ee_ge::um::{um_arccos, um_arctan2};

use crate::effect_rising::{EeGfxEffect3DOp, EeGfxGraphicFacts, Effect3DHost};
use crate::effect_state::{identity, load_point, save_point, EeGfxEffectRecord, EffectFile};
use crate::effects::EeGfxColor;
use crate::lighting::EeGfxLighting;
use crate::raster::{Raster, BLEND_INV_SRC_ALPHA, BLEND_ONE, BLEND_SRC_ALPHA, ZBUFFER_TEST_ONLY};

/// The tracers, the shells (a tracer with a pass-5 draw) and the pulse cannon ball.
pub const EE_GFX_EFFECT_KIND_TRACERS: i32 = 0x21;
pub const EE_GFX_EFFECT_KIND_SHELLS: i32 = 0x26;
pub const EE_GFX_EFFECT_KIND_PULSE_CANNON_BALL: i32 = 0x43;

/// `kErrorOK`.
const OK: i32 = 0;

/// A shot's state.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeGfxEffectShot {
    /// `mPath`: from the weapon point to the end (saved)
    pub path: Ge3DLine,
    /// `mLocation`: the place the draw uses (`Queue` copies it); the end while constructing
    pub location: Ge3DPoint,
    /// `mColor`: the shooter's player colour (the tracers')
    pub color: EeGfxColor,
    /// the ball's `mScale` and its glow's `mGlowScale` (saved) and the draw's copies
    pub scale: f32,
    pub draw_scale: f32,
    pub glow_scale: f32,
    pub draw_glow_scale: f32,
    /// `mPosition`: 0..1 along the path (saved; the ball goes on to 1.5) and the draw's copy
    pub position: f32,
    pub draw_position: f32,
    /// `mSpeed`: path per millisecond (saved)
    pub speed: f32,
    /// the citizen beam's (`effect_beam.rs`): `mEnd` (saved), `mMiddle` (saved), `mF9C` (saved, never set),
    /// `mActive` (saved) and the draw's copy
    pub end: Ge3DPoint,
    pub middle: Ge3DPoint,
    pub unused: f32,
    pub active: bool,
    pub draw_active: bool,
}

/// Whether `kind` is a shot.
pub fn is_shot(kind: i32) -> bool {
    matches!(kind, EE_GFX_EFFECT_KIND_TRACERS | EE_GFX_EFFECT_KIND_SHELLS | EE_GFX_EFFECT_KIND_PULSE_CANNON_BALL)
}

/// The shots' virtuals.
///
/// # Safety
/// As `effect_rising::effect3d`.
pub(crate) unsafe fn shot<H: Effect3DHost, R: Raster>(
    op: EeGfxEffect3DOp,
    r: *mut EeGfxEffectRecord,
    g: &EeGfxGraphicFacts,
    h: &mut H,
    x: &mut R,
) -> i32 {
    // SAFETY (every access below): the function's contract; no reference to the record is kept
    let kind = unsafe { (*r).kind };
    let ball = kind == EE_GFX_EFFECT_KIND_PULSE_CANNON_BALL;
    match op {
        EeGfxEffect3DOp::Create => {
            // the C++ put the weapon point in the path's start and the end in the location (and a tracer's colour)
            let (start, end) = unsafe { ((*r).shot.path.start, (*r).shot.location) };
            let mut direction = GeVector { x: end.x - start.x, y: end.y - start.y, z: end.z - start.z };
            unsafe {
                let s = &mut (*r).shot;
                s.location = Ge3DPoint::default();
                s.position = 0.0;
                s.draw_position = 0.0;
                s.path.direction = direction;
            }
            direction.normalize();
            let yaw = um_arctan2(-direction.x, direction.y);
            let mut pitch = um_arccos(-direction.z);
            pitch -= 1.5707964;
            let mut t = identity();
            t.m[0][3] = start.x;
            t.m[1][3] = start.y;
            t.m[2][3] = start.z;
            t.set_orientation(ee_ge::transform::RotationOrder::Ypr, yaw, pitch, 0.0);
            unsafe { (*r).transformation = t };
            h.move_to();
            unsafe {
                (*r).bounding_radius = g.size;
                let s = &mut (*r).shot;
                let duration = g.duration as f32 * 0.00001;
                s.speed = duration / s.path.direction.magnitude();
                if ball {
                    s.glow_scale = 0.1;
                    s.scale = 1.0;
                }
            }
            0
        }
        EeGfxEffect3DOp::SetUp => h.load_media(),
        EeGfxEffect3DOp::Queue => {
            let result = h.base_queue();
            if result == 0 {
                unsafe {
                    let t = (*r).transformation;
                    let s = &mut (*r).shot;
                    s.draw_position = s.position;
                    if ball {
                        s.draw_scale = s.scale;
                        s.draw_glow_scale = s.glow_scale;
                    }
                    s.location = Ge3DPoint { x: t.m[0][3], y: t.m[1][3], z: t.m[2][3] };
                }
                h.queue(4);
            }
            result
        }
        EeGfxEffect3DOp::DrawPass6 => {
            let (mut t, s) = unsafe { ((*r).transformation, (*r).shot) };
            let mut glow_t = t;
            let model = h.model();
            let glow = if ball { h.model_at(1) } else { std::ptr::null_mut() };
            let scale = g.size;
            x.set_alpha_ref(if g.has_object { g.alpha_ref } else { 0.0 });
            let fog = x.get_fog();
            x.set_zbuffer(ZBUFFER_TEST_ONLY);
            x.set_blending(BLEND_ONE, BLEND_ONE);
            x.set_shading_source(0);
            x.lighting(EeGfxLighting::White);
            if ball || !g.flag1 {
                x.model_texture(model, h.texture());
            } else {
                x.model_color(model, s.color);
            }
            let location = s.location;
            let value = x.fog_factor(&location, fog);
            t.m[0][3] = location.x;
            t.m[1][3] = location.y;
            t.m[2][3] = location.z;
            x.model_transform(model, &t);
            let result = if ball {
                x.model_scale(model, scale * s.draw_scale);
                x.set_ambient(EeGfxColor { red: value, green: value, blue: value });
                x.model_prepare(model);
                x.model_draw(model, 1.0);
                glow_t.m[0][3] = location.x;
                glow_t.m[1][3] = location.y;
                glow_t.m[2][3] = location.z;
                x.model_transform(glow, &glow_t);
                x.model_scale(glow, scale * s.draw_glow_scale);
                x.model_texture(glow, h.texture());
                x.model_prepare(glow);
                x.model_draw(glow, 1.0)
            } else {
                x.model_scale(model, scale);
                let light = if s.draw_position >= 0.5 { (1.0 - s.draw_position) * value * 2.0 } else { value };
                x.set_ambient(EeGfxColor { red: light, green: light, blue: light });
                x.model_prepare(model);
                x.model_draw(model, 1.0)
            };
            x.set_fog(fog);
            result
        }
        EeGfxEffect3DOp::DrawPass5 if kind == EE_GFX_EFFECT_KIND_SHELLS => {
            let (mut t, s) = unsafe { ((*r).transformation, (*r).shot) };
            let model = h.model();
            x.set_alpha_ref(if g.has_object { g.alpha_ref } else { 0.0 });
            x.set_zbuffer(ZBUFFER_TEST_ONLY);
            x.set_blending(BLEND_SRC_ALPHA, BLEND_INV_SRC_ALPHA);
            x.set_shading_source(0);
            x.lighting(EeGfxLighting::Unit);
            x.model_texture(model, h.texture());
            t.m[0][3] = s.location.x;
            t.m[1][3] = s.location.y;
            t.m[2][3] = s.location.z;
            x.model_transform(model, &t);
            x.model_scale(model, g.size);
            x.model_opacity(model, if s.draw_position >= 0.5 { (1.0 - s.draw_position) * 2.0 } else { 1.0 });
            x.model_prepare(model);
            let result = x.model_draw(model, 1.0);
            x.model_opacity(model, 1.0);
            result
        }
        EeGfxEffect3DOp::Step => {
            let position = unsafe {
                let s = &mut (*r).shot;
                s.position += g.tick_time * s.speed;
                s.position
            };
            if position <= 1.0 {
                unsafe {
                    let point = (*r).shot.path.point_at(position);
                    (*r).transformation.m[0][3] = point.x;
                    (*r).transformation.m[1][3] = point.y;
                    (*r).transformation.m[2][3] = point.z;
                }
                h.move_to();
                if ball {
                    unsafe { pulse(&mut (*r).shot) };
                }
                0
            } else if ball {
                if position < 1.5 {
                    unsafe { explode(&mut (*r).shot) };
                    0
                } else {
                    7
                }
            } else {
                h.shot_hit();
                7
            }
        }
        _ => 0,
    }
}

/// The ball's pulse along the way: 1.25 in the odd tenths past the first, 1 in the even ones (the glow back to 0.1
/// in the second tenth).
fn pulse(s: &mut EeGfxEffectShot) {
    let p = s.position;
    if p > 0.9 {
        s.scale = 1.25;
    } else if p > 0.8 {
        s.scale = 1.0;
    } else if p > 0.7 {
        s.scale = 1.25;
    } else if p > 0.6 {
        s.scale = 1.0;
    } else if p > 0.5 {
        s.scale = 1.25;
    } else if p > 0.4 {
        s.scale = 1.0;
    } else if p > 0.3 {
        s.scale = 1.25;
    } else if p > 0.2 {
        s.scale = 1.0;
    } else if p > 0.1 {
        s.scale = 1.25;
        s.glow_scale = 0.1;
    }
}

/// The ball's explosion past the end: growing to 32 by 1.4, its glow up to 1.5 and back to 0.1.
fn explode(s: &mut EeGfxEffectShot) {
    let p = s.position;
    if p > 1.4 {
        s.scale = 32.0;
        s.glow_scale = 0.1;
    } else if p > 1.3 {
        s.scale = 16.0;
        s.glow_scale = 0.5;
    } else if p > 1.2 {
        s.scale = 8.0;
        s.glow_scale = 1.0;
    } else if p > 1.1 {
        s.scale = 4.0;
        s.glow_scale = 1.5;
    } else {
        s.scale = 2.0;
        s.glow_scale = 2.0;
    }
}

/// `GE3DLine::Load`: the start, then the direction (`GEVector::Load`, the same reads).
pub(crate) fn load_line<F: EffectFile>(f: &mut F, line: &mut Ge3DLine) -> i32 {
    let mut res = load_point(f, &mut line.start);
    if res == OK {
        let d = line.direction;
        let mut p = Ge3DPoint { x: d.x, y: d.y, z: d.z };
        res = load_point(f, &mut p);
        line.direction = GeVector { x: p.x, y: p.y, z: p.z };
    }
    res
}

/// `GE3DLine::Save`.
pub(crate) fn save_line<F: EffectFile>(f: &mut F, line: &Ge3DLine) -> i32 {
    let mut res = save_point(f, &line.start);
    if res == OK {
        let d = line.direction;
        res = save_point(f, &Ge3DPoint { x: d.x, y: d.y, z: d.z });
    }
    res
}

/// A shot's own Load after the base's and the media load (both ignored): the position, speed, path and, the ball's,
/// both scales; the first error.
pub fn load_shot<F: EffectFile>(r: &mut EeGfxEffectRecord, f: &mut F) -> i32 {
    let ball = r.kind == EE_GFX_EFFECT_KIND_PULSE_CANNON_BALL;
    let s = &mut r.shot;
    let mut res = f.read_f32(&mut s.position);
    if res == OK {
        res = f.read_f32(&mut s.speed);
    }
    if res == OK {
        res = load_line(f, &mut s.path);
    }
    if ball {
        if res == OK {
            res = f.read_f32(&mut s.scale);
        }
        if res == OK {
            res = f.read_f32(&mut s.glow_scale);
        }
    }
    res
}

/// A shot's own Save after the base's (ignored).
pub fn save_shot<F: EffectFile>(r: &EeGfxEffectRecord, f: &mut F) -> i32 {
    let s = &r.shot;
    let mut res = f.write_f32(s.position);
    if res == OK {
        res = f.write_f32(s.speed);
    }
    if res == OK {
        res = save_line(f, &s.path);
    }
    if r.kind == EE_GFX_EFFECT_KIND_PULSE_CANNON_BALL {
        if res == OK {
            res = f.write_f32(s.scale);
        }
        if res == OK {
            res = f.write_f32(s.glow_scale);
        }
    }
    res
}

#[cfg(test)]
#[path = "effect_shot_tests.rs"]
mod tests;
