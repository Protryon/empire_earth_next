//! The blast wave (kind 0x18) and the shield (kind 0x45) whole in Rust (`EEE3DBlastWave`, `EEE3DShield`, deleted):
//! models that follow their unit's first tag point of type 0x12 (`FollowUnit`, the C++'s host call). The blast wave
//! grows (or, flag 1, shrinks) additively over its duration and dims over its second half; the shield scrolls its
//! texture between params 2 and 3, flashes to param 6 when its unit loses hit points and fades towards param 5,
//! fading out over its last tenth. Both on `EEE3DRust` like `effect_rising.rs`.
//!
//! The record is reached through its raw pointer field by field, as there: the C++ reads it in the host calls.

use ee_ge::types::Ge3DPoint;

use crate::effect_rising::{EeGfxEffect3DOp, EeGfxGraphicFacts, Effect3DHost};
use crate::effect_state::{
    identity, timed_step, EeGfxEffectRecord, EeGfxTimedTick, EffectFile, EE_GFX_EFFECT_KIND_BLAST_WAVE,
};
use crate::effects::EeGfxColor;
use crate::lighting::EeGfxLighting;
use crate::raster::{
    Raster, BLEND_INV_SRC_ALPHA, BLEND_ONE, BLEND_SRC_ALPHA, ZBUFFER_TEST_AND_WRITE, ZBUFFER_TEST_ONLY,
};

/// The shield.
pub const EE_GFX_EFFECT_KIND_SHIELD: i32 = 0x45;

/// The blast wave's and shield's smallest scale.
const MIN_SCALE: f32 = 0.1;

/// `max(a, b)` as the Windows macro: `a > b ? a : b`.
fn win_max(a: f32, b: f32) -> f32 {
    if a > b {
        a
    } else {
        b
    }
}

/// The blast wave's and shield's virtuals.
///
/// # Safety
/// As `effect_rising::effect3d`.
pub(crate) unsafe fn follow<H: Effect3DHost, R: Raster>(
    op: EeGfxEffect3DOp,
    r: *mut EeGfxEffectRecord,
    g: &EeGfxGraphicFacts,
    h: &mut H,
    x: &mut R,
) -> i32 {
    // SAFETY (every access below): the function's contract; no reference to the record is kept
    let blast = unsafe { (*r).kind } == EE_GFX_EFFECT_KIND_BLAST_WAVE;
    match op {
        EeGfxEffect3DOp::Create => {
            if blast {
                unsafe {
                    (*r).aged.aging = true;
                    (*r).aged.age = 0.0;
                    (*r).aged.draw_age = 0.0;
                    (*r).bounding_radius = g.size;
                }
            }
            0
        }
        EeGfxEffect3DOp::CreateAt => {
            // the blast wave at a point: the C++ put the point in the record's location
            unsafe {
                let p = (*r).aged.location;
                let mut t = identity();
                t.m[0][3] = p.x;
                t.m[1][3] = p.y;
                t.m[2][3] = p.z;
                (*r).aged.aging = true;
                (*r).aged.age = 0.0;
                (*r).aged.draw_age = 0.0;
                (*r).aged.location = Ge3DPoint::default();
                (*r).bounding_radius = g.size;
                (*r).transformation = t;
            }
            h.move_to();
            0
        }
        EeGfxEffect3DOp::SetUp => {
            let result = h.load_media();
            if blast {
                if g.has_unit {
                    h.follow_unit();
                }
            } else {
                unsafe {
                    (*r).aged.age = 0.0;
                    let s = &mut (*r).shield;
                    s.hit_points = g.unit_hit_points;
                    s.draw_opacity = g.param5;
                    s.opacity = g.param5;
                    s.scrolling_down = false;
                    s.scroll = 0.0;
                    s.draw_scroll = 0.0;
                }
                h.follow_unit();
                unsafe { (*r).bounding_radius = g.size };
            }
            result
        }
        EeGfxEffect3DOp::Queue => {
            let result = h.base_queue();
            if result == 0 {
                unsafe {
                    (*r).aged.draw_age = (*r).aged.age;
                    if !blast {
                        (*r).shield.draw_opacity = (*r).shield.opacity;
                        (*r).shield.draw_scroll = (*r).shield.scroll;
                        let t = (*r).transformation;
                        (*r).aged.location = Ge3DPoint { x: t.m[0][3], y: t.m[1][3], z: t.m[2][3] };
                    }
                }
                h.queue(if blast { 4 } else { 3 });
            }
            result
        }
        EeGfxEffect3DOp::DrawPass6 if blast => {
            let t = unsafe { (*r).transformation };
            let draw_age = unsafe { (*r).aged.draw_age };
            let model = h.model();
            let mut scale = g.size;
            x.set_alpha_ref(if g.has_object { g.alpha_ref } else { 0.0 });
            let fog = x.get_fog();
            let mut fade = 1.0 - draw_age;
            fade = fade * fade;
            if g.flag1 {
                scale = fade * fade * scale;
                if scale < MIN_SCALE {
                    scale = MIN_SCALE;
                }
            } else {
                scale = win_max(MIN_SCALE, 1.0 - fade * fade) * scale;
            }
            x.set_zbuffer(ZBUFFER_TEST_ONLY);
            x.set_blending(BLEND_ONE, BLEND_ONE);
            x.set_shading_source(0);
            x.lighting(EeGfxLighting::White);
            x.model_texture(model, h.texture());
            let location = Ge3DPoint { x: t.m[0][3], y: t.m[1][3], z: t.m[2][3] };
            let value = x.fog_factor(&location, fog);
            x.model_transform(model, &t);
            x.model_scale(model, scale);
            let light = if draw_age >= 0.5 { (1.0 - draw_age) * value * 2.0 } else { value };
            x.set_ambient(EeGfxColor { red: light, green: light, blue: light });
            x.model_prepare(model);
            let result = x.model_draw(model, 1.0);
            x.set_fog(fog);
            result
        }
        EeGfxEffect3DOp::DrawPass5 if !blast => {
            let (location, draw_age, s) = unsafe { ((*r).aged.location, (*r).aged.draw_age, (*r).shield) };
            let model = h.model();
            x.set_alpha_ref(if g.has_object { g.alpha_ref } else { 0.0 });
            x.set_zbuffer(ZBUFFER_TEST_AND_WRITE);
            x.set_blending(BLEND_SRC_ALPHA, BLEND_INV_SRC_ALPHA);
            x.set_shading_source(0);
            x.lighting(EeGfxLighting::Unit);
            let mut t = identity();
            t.m[0][3] = location.x;
            t.m[1][3] = location.y;
            t.m[2][3] = location.z;
            x.model_transform(model, &t);
            x.model_scale(model, g.size);
            x.model_scroll_v(model, s.draw_scroll);
            x.model_prepare(model);
            x.model_texture(model, h.texture());
            x.model_opacity(model, if draw_age >= 0.9 { (1.0 - draw_age) * 10.0 } else { s.draw_opacity });
            let result = x.model_draw(model, 1.0);
            x.model_opacity(model, 1.0);
            x.model_restore_v(model);
            result
        }
        EeGfxEffect3DOp::Step if blast => {
            if g.has_unit {
                h.follow_unit();
            }
            let tick = EeGfxTimedTick {
                tick_seconds: g.tick_seconds,
                tick_time: g.tick_time,
                duration: g.duration,
                speed: 0.0,
                sinking: false,
            };
            let mut local = unsafe { r.read() };
            let result = timed_step(&mut local, &tick);
            unsafe { r.write(local) };
            result
        }
        EeGfxEffect3DOp::Step => {
            if g.unit_hit_points <= 0 {
                return 7;
            }
            let elapsed = g.tick_time;
            let age = unsafe {
                (*r).aged.age += elapsed / g.duration as f32;
                let s = &mut (*r).shield;
                if s.scrolling_down {
                    if g.param2 > s.scroll {
                        s.scrolling_down = false;
                    }
                } else if g.param3 < s.scroll {
                    s.scrolling_down = true;
                }
                if s.scrolling_down {
                    s.scroll -= g.param1 * elapsed;
                } else {
                    s.scroll += g.param1 * elapsed;
                }
                (*r).aged.age
            };
            if age <= 1.0 {
                h.follow_unit();
                unsafe {
                    let s = &mut (*r).shield;
                    s.opacity = if s.hit_points > g.unit_hit_points {
                        g.param6
                    } else {
                        win_max(g.param5, s.opacity - g.param4)
                    };
                    s.hit_points = g.unit_hit_points;
                }
                0
            } else {
                7
            }
        }
        _ => 0,
    }
}

/// The shield's own Load after the base's and the media load (both ignored): the age, opacity, hit points, scroll
/// and, from version 0.180, the scroll direction; the first error.
pub fn load_shield<F: EffectFile>(r: &mut EeGfxEffectRecord, f: &mut F, major: u32, minor: u32) -> i32 {
    let mut res = f.read_f32(&mut r.aged.age);
    let s = &mut r.shield;
    if res == 0 {
        res = f.read_f32(&mut s.opacity);
    }
    if res == 0 {
        res = f.read_i32(&mut s.hit_points);
    }
    if res == 0 {
        res = f.read_f32(&mut s.scroll);
    }
    if res == 0 && (major > 0 || minor >= 0xb4) {
        res = f.read_bool(&mut s.scrolling_down);
    }
    res
}

/// The shield's own Save after the base's (ignored).
pub fn save_shield<F: EffectFile>(r: &EeGfxEffectRecord, f: &mut F) -> i32 {
    let s = &r.shield;
    let mut res = f.write_f32(r.aged.age);
    if res == 0 {
        res = f.write_f32(s.opacity);
    }
    if res == 0 {
        res = f.write_i32(s.hit_points);
    }
    if res == 0 {
        res = f.write_f32(s.scroll);
    }
    if res == 0 {
        res = f.write_bool(s.scrolling_down);
    }
    res
}

#[cfg(test)]
#[path = "effect_follow_tests.rs"]
mod tests;
