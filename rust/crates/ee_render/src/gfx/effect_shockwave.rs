//! The 2D shockwave (kind 0x12) whole in Rust on `EEE2DRust` (`EEE2DShockwave`, 0xe6192..0xe6600, deleted): an
//! additive camera-facing sprite on a unit (at a random one of its kind 4 tag points, or at the unit) or at a point,
//! swelling from the effect's start size to its size over the duration (eased in under flag 1, else out) and fading
//! over its second half. Its age is the aged record's (`effect_state::aged_step`, `load_aged`, `save_aged`); the C++
//! host places it (`spawn_point`), loads the media (`object_media`) and keeps `EEEffect2D`'s queue, Load and Save.

use crate::ge::transform::RotationOrder;
use crate::ge::types::{Ge3DPoint, GeTransformation};

use crate::gfx::effect_2d::Effect2DHost;
use crate::gfx::effect_rising::{EeGfxEffect3DOp, EeGfxGraphicFacts};
use crate::gfx::effect_state::{aged_step, load_aged, save_aged, EeGfxEffectRecord, EffectFile};
use crate::gfx::effects::EeGfxColor;
use crate::gfx::lighting::EeGfxLighting;
use crate::gfx::raster::{Raster, BLEND_ONE, ZBUFFER_TEST_ONLY};

/// The shockwave's virtuals.
///
/// # Safety
/// As `effect_2d::effect2d`.
pub unsafe fn shockwave<H: Effect2DHost, R: Raster>(
    op: EeGfxEffect3DOp,
    r: *mut EeGfxEffectRecord,
    g: &EeGfxGraphicFacts,
    h: &mut H,
    x: &mut R,
) -> i32 {
    // SAFETY (every access below): the function's contract; no reference to the record is kept across a host call
    match op {
        // 0xe6192 on a unit
        EeGfxEffect3DOp::Create => {
            unsafe { (*r).aged.draw_age = 0.0 };
            0
        }
        // 0xe61e2 at a point (the C++ put it in the record's `sprite.start`): `SetPosition`
        EeGfxEffect3DOp::CreateAt => {
            unsafe {
                (*r).aged.draw_age = 0.0;
                (*r).position = (*r).sprite.start;
            }
            h.move_to_point();
            0
        }
        // 0xe6259: a unit's at one of its kind 4 tag points; the radius; the media
        EeGfxEffect3DOp::SetUp => {
            if g.has_unit {
                let p = h.spawn_point(4);
                unsafe { (*r).position = p };
                h.move_to_point();
            }
            unsafe { (*r).bounding_radius = g.size };
            h.object_media()
        }
        // `EEEffect2D::Queue`, then the draw copy and render queue 4
        EeGfxEffect3DOp::Queue => {
            let result = h.base_queue();
            if result == 0 {
                unsafe { (*r).aged.draw_age = (*r).aged.age };
                h.queue(4);
            }
            result
        }
        // 0xe6383
        EeGfxEffect3DOp::DrawPass6 => {
            let model = h.model();
            x.set_alpha_ref(if g.has_object { g.alpha_ref } else { 0.0 });
            let fog = x.get_fog();
            let age = unsafe { (*r).aged.draw_age };
            let fade = (1.0 - age) * (1.0 - age);
            let factor = if g.flag1 { fade * fade } else { 1.0 - fade * fade };
            x.set_zbuffer(ZBUFFER_TEST_ONLY);
            x.set_blending(BLEND_ONE, BLEND_ONE);
            x.set_shading_source(0);
            x.lighting(EeGfxLighting::White);
            let texture = h.object_texture(0);
            x.model_texture(model, texture);
            let mut t = GeTransformation::default();
            t.set_orientation(RotationOrder::Ypr, x.camera_yaw(), 0.0, 0.0);
            let p: Ge3DPoint = unsafe { (*r).position };
            t.m[0][3] = p.x;
            t.m[1][3] = p.y;
            t.m[2][3] = p.z;
            t.m[3] = [0.0, 0.0, 0.0, 1.0];
            x.model_transform(model, &t);
            x.model_scale(model, (g.size - g.start_size) * factor + g.start_size);
            let value = x.fog_factor(&p, fog);
            let light = if age < 0.5 { value } else { (1.0 - age) * value * 2.0 };
            x.set_ambient(EeGfxColor { red: light, green: light, blue: light });
            x.model_prepare(model);
            let result = x.model_draw(model, 1.0);
            x.set_fog(fog);
            result
        }
        // 0xe65e2
        EeGfxEffect3DOp::Step => {
            let mut tile = [0; 2];
            let mut local = unsafe { r.read() };
            let result = aged_step(&mut local, g.tick_seconds, g.duration, &mut tile);
            unsafe { r.write(local) };
            result
        }
        _ => 0,
    }
}

/// `Load` (0xe6585: the media again, its result ignored) and `Save` (0xe65bc) after `EEEffect2D`'s, whose result both
/// ignore: the age.
pub fn shockwave_file<H: Effect2DHost, F: EffectFile>(
    load: bool,
    r: &mut EeGfxEffectRecord,
    f: &mut F,
    h: &mut H,
) -> i32 {
    if load {
        h.object_media();
        load_aged(r, f)
    } else {
        save_aged(r, f)
    }
}

#[cfg(test)]
#[path = "effect_shockwave_tests.rs"]
mod tests;
