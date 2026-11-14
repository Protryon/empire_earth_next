//! The hurricane (kind 0x24) whole in Rust (`EEE3DHurricane`, deleted): the graphic's model on a unit playing its
//! animation 13 with the animation's sound (the disease's shape, `effect_orbit.rs`), its orbiting parts turned, a
//! scale growing in from a tenth over the first 30% of the effect's duration and shrinking back after its unit dies
//! (7 at a tenth), and the graphic effect's linked effect 1 placed every so often (int params 1 and 2: the mean and
//! the spread, ms) at a random tag point of kind 6 of the graphic around the unit. Drawn in pass 6 with clockwise
//! culling (restored after), z writes, alpha blended and unit lit.
//!
//! `EEE3DRust` holds the graphic object and the sound and answers the world (`EeGfxEffect3DHost`); the draws go
//! through the rasterizer table (`raster.rs`). The record is reached through its raw pointer field by field: the C++
//! reads it during the host calls (the region move), so no Rust reference to it lives across one.

use ee_ge::um::lrintf;

use crate::effect_orbit::{copy_orbitals, keep_angles, turn_orbitals, update_orbitals};
use crate::effect_rising::{EeGfxEffect3DOp, EeGfxGraphicFacts, Effect3DHost};
use crate::effect_state::{identity, random, EeGfxEffectRecord, EffectFile};
use crate::lighting::EeGfxLighting;
use crate::raster::{Raster, BLEND_INV_SRC_ALPHA, BLEND_SRC_ALPHA, CULL_CW, ZBUFFER_TEST_AND_WRITE};

/// `kEffect3DHurricane`.
pub const EE_GFX_EFFECT_KIND_HURRICANE: i32 = 0x24;

/// `kErrorOK`.
const OK: i32 = 0;
/// The scale it starts at and shrinks back to.
const LEAST: f32 = 0.1;

/// The hurricane's virtuals.
///
/// # Safety
/// As `effect_rising::effect3d`.
pub(crate) unsafe fn hurricane<H: Effect3DHost, R: Raster>(
    op: EeGfxEffect3DOp,
    r: *mut EeGfxEffectRecord,
    g: &EeGfxGraphicFacts,
    h: &mut H,
    x: &mut R,
) -> i32 {
    // SAFETY (every access below): the function's contract; no reference to the record is kept across a host call
    match op {
        EeGfxEffect3DOp::SetUp => {
            let result = h.load_media();
            let animation = h.animation();
            unsafe {
                let o = &mut (*r).orbit;
                o.frame = animation.start;
                o.time = 0.0;
                o.spawn_time = 0.0;
                o.draw_frame = 0;
                o.scale = LEAST;
                copy_orbitals(r, h, false);
            }
            let location = h.unit_location();
            unsafe { (*r).transformation = location };
            h.move_to();
            unsafe { (*r).bounding_radius = g.size };
            result
        }
        EeGfxEffect3DOp::Queue => {
            let result = h.base_queue();
            if result == 0 {
                unsafe {
                    let o = &mut (*r).orbit;
                    o.draw_frame = o.frame;
                    o.draw_scale = o.scale;
                    let t = (*r).transformation;
                    (*r).aged.location = ee_ge::types::Ge3DPoint { x: t.m[0][3], y: t.m[1][3], z: t.m[2][3] };
                    keep_angles(r);
                }
                h.queue(4);
            }
            result
        }
        EeGfxEffect3DOp::DrawPass6 => unsafe { draw(r, g, h, x) },
        EeGfxEffect3DOp::Step => unsafe { step(r, g, h) },
        _ => 0,
    }
}

/// The step: 7 once its unit is dead and it has shrunk to a tenth, else 0 or the linked effect's placement result.
///
/// # Safety
/// As `hurricane`.
unsafe fn step<H: Effect3DHost>(r: *mut EeGfxEffectRecord, g: &EeGfxGraphicFacts, h: &mut H) -> i32 {
    let mut result = 0;
    // SAFETY (every access below): the function's contract
    let (old, mut time) = unsafe { ((*r).orbit.frame, (*r).orbit.time) };
    let mut frame = old;
    let elapsed = g.tick_time;
    let animation = h.animation();
    let start = frame == animation.start && time == 0.0;
    if g.unit_hit_points == 0 {
        unsafe {
            let o = &mut (*r).orbit;
            if o.scale <= LEAST {
                result = 7;
            } else {
                let shrunk = o.scale - elapsed / 2000.0;
                o.scale = if LEAST > shrunk { LEAST } else { shrunk };
            }
        }
        return result;
    }
    unsafe {
        (*r).aged.age += elapsed / g.duration as f32;
        let o = &mut (*r).orbit;
        if (*r).aged.age < 0.3 {
            let grown = elapsed / 2000.0 + o.scale;
            o.scale = if 1.0 < grown { 1.0 } else { grown };
        } else {
            o.scale = 1.0;
        }
    }
    let location = h.unit_location();
    unsafe { (*r).transformation = location };
    h.move_to();
    time = elapsed + time;
    let spawn = unsafe {
        (*r).orbit.spawn_time -= elapsed;
        (*r).orbit.spawn_time
    };
    if spawn <= 0.0 && g.has_linked1 {
        let point = spawn_point(h, old);
        result = h.spawn(&point);
        let spread = (0.5 - random()) * g.int_param2 as f32 + g.int_param1 as f32;
        unsafe { (*r).orbit.spawn_time = spread };
    }
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
    result
}

/// `GetSpawnPoint`: a random tag point of kind 6 of the graphic at `frame` around the unit (scaled by the graphic
/// effect's size); the origin when the graphic has none (no draw then).
fn spawn_point<H: Effect3DHost>(h: &mut H, frame: u32) -> ee_ge::types::Ge3DPoint {
    let count = h.spawn_count();
    if count == 0 {
        return ee_ge::types::Ge3DPoint::default();
    }
    let mut index = lrintf(random() * count as f32) as u32;
    if index == count {
        index = count - 1;
    }
    h.spawn_at(index, frame)
}

/// The pass-6 draw.
///
/// # Safety
/// As `hurricane`.
unsafe fn draw<H: Effect3DHost, R: Raster>(
    r: *mut EeGfxEffectRecord,
    g: &EeGfxGraphicFacts,
    h: &mut H,
    x: &mut R,
) -> i32 {
    let model = h.model();
    let culling = x.get_culling();
    x.set_alpha_ref(if g.has_object { g.alpha_ref } else { 0.0 });
    x.set_culling(CULL_CW);
    x.set_zbuffer(ZBUFFER_TEST_AND_WRITE);
    x.set_blending(BLEND_SRC_ALPHA, BLEND_INV_SRC_ALPHA);
    x.set_shading_source(0);
    x.lighting(EeGfxLighting::Unit);
    // SAFETY (every access below): the function's contract
    let (location, draw_frame, draw_scale) =
        unsafe { ((*r).aged.location, (*r).orbit.draw_frame, (*r).orbit.draw_scale) };
    let mut t = identity();
    t.m[0][3] = location.x;
    t.m[1][3] = location.y;
    t.m[2][3] = location.z;
    x.model_transform(model, &t);
    x.model_frame(model, draw_frame);
    x.model_scale(model, g.size * draw_scale);
    unsafe { turn_orbitals(r, x) };
    let texture = h.texture();
    x.model_own_texture(model, texture);
    x.model_prepare(model);
    let result = x.model_draw(model, 1.0);
    x.set_culling(culling);
    result
}

/// `Load` after the base's (ignored): the media and the orbitals (both results ignored), then the time, frame, spawn
/// timer, age and scale; the first read error.
///
/// # Safety
/// `r` is a live record.
pub(crate) unsafe fn load_hurricane<H: Effect3DHost, F: EffectFile>(
    r: *mut EeGfxEffectRecord,
    f: &mut F,
    h: &mut H,
) -> i32 {
    h.load_media();
    // SAFETY (every access below): the function's contract
    unsafe {
        copy_orbitals(r, h, false);
        let o = &mut (*r).orbit;
        let mut res = f.read_f32(&mut o.time);
        if res == OK {
            res = f.read_u32(&mut o.frame);
        }
        if res == OK {
            res = f.read_f32(&mut o.spawn_time);
        }
        if res == OK {
            res = f.read_f32(&mut (*r).aged.age);
        }
        if res == OK {
            res = f.read_f32(&mut (*r).orbit.scale);
        }
        res
    }
}

/// `Save` after the base's (ignored): the time, frame, spawn timer, age and scale; the first write error.
pub(crate) fn save_hurricane<F: EffectFile>(r: &EeGfxEffectRecord, f: &mut F) -> i32 {
    let o = &r.orbit;
    let mut res = f.write_f32(o.time);
    if res == OK {
        res = f.write_u32(o.frame);
    }
    if res == OK {
        res = f.write_f32(o.spawn_time);
    }
    if res == OK {
        res = f.write_f32(r.aged.age);
    }
    if res == OK {
        res = f.write_f32(o.scale);
    }
    res
}

#[cfg(test)]
#[path = "effect_hurricane_tests.rs"]
mod tests;
